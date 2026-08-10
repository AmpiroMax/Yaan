/*
Created: 09:08:2026 - 00:45:00
Last updated: 10:08:2026 - 10:28:59
Module: engine/app
File: engine/app/sources/App.h

Responsibility:
- The composition root: owns every subsystem, runs the main loop (fixed-step
  simulation + interpolated render, Rule 12), ferries chunk events from world
  to render/physics (they are DAG siblings and cannot include each other).

Key items:
- AppConfig: backend selection + window/internal resolution (env-overridable).
- App: init() wires backends; run() is the loop; shutdown() tears down.

Dependencies:
- Uses: all platform interfaces, core (ecs/time/events), world, render, gameplay.
- Used by: main.cpp only (Rule 22).

Notes:
- The movement-system seam (sim zone) is integrated via gameplay's fixed-tick
  API; see App.cpp integration notes.
- Tour (Q51): when DFN_TOUR=1 the tour overrides the camera and the app closes
  after the last screenshot.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. LEAD-owned file (Rule 25).
*/
/*
UPD:
- 09:08:2026 - 00:45:00: Created for stage 2 integration.
- 09:08:2026 - 00:48:00: Adopted sim's free-function movement API (Rule 9) —
                         player entity id held here, no system object.
- 09:08:2026 - 10:48:00: palette_post config flag (Q9b, stage-3 render batch).
- 09:08:2026 - 12:49:12: Graphics settings file (user decision, sync #3): settings.cfg
                         read at startup, auto-generated on first run; env
                         overrides file for tooling.
- 09:08:2026 - 17:16:27: world_edge_: static walls at the generated extent.
- 09:08:2026 - 19:12:24: game clock (day/night cycle) held here.
- 09:08:2026 - 22:24:44: Игровые часы стартуют с START_TIME_OF_DAY, а не с нуля — ноль это полночь, и свежий запуск открывался в темноте.
- 10:08:2026 - 02:39:07: Audio, step context, first-person body rig and the autonomous playtest join the composition root (landscape stage wiring).
- 10:08:2026 - 10:52:00: BodyProbe (DFN_BODY_PROBE) — the acceptance-frame path
                         for anything ANIMATED. The Tour freezes the tick, so it
                         can photograph only still life; this probe runs the
                         world and triggers the shot off simulation state.
- 10:08:2026 - 10:28:59: Menu-first launch: init() raises the engine, enter_world() builds a chosen demo map (user request: check different maps, with and without the menu).
*/

#pragma once

#include "engine/anim/sources/Rig.h"
#include "engine/app/sources/Menu.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/core/events/sources/EventBus.h"
#include "engine/core/time/sources/FixedTimestep.h"
#include "engine/gameplay/sources/PlayerMovement.h"
#include "engine/gameplay/sources/PlaytestBot.h"
#include "engine/gameplay/sources/StepAudio.h"
#include "engine/platform/audio/interfaces/IAudio.h"
#include "engine/platform/physics/interfaces/IPhysics.h"
#include "engine/render/sources/FirstPersonCamera.h"
#include "engine/render/sources/RenderSystem.h"
#include "engine/render/sources/Tour.h"
#include "engine/world/sources/ChunkManager.h"

#include <array>
#include <optional>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace dfn::platform {
class IWindow;
class IInput;
class IRenderer;
class IPhysics;
} // namespace dfn::platform

namespace dfn::app {

struct AppConfig {
    uint32_t window_width = 1280;
    uint32_t window_height = 720;
    uint32_t internal_width = 0;  // 0 = take dfn::config INTERNAL_RES_W
    uint32_t internal_height = 0; // 0 = take dfn::config INTERNAL_RES_H
    bool use_null_renderer = false;
    bool use_null_physics = false;
    bool use_null_audio = false;   // DFN_NULL_AUDIO=1
    bool show_menu = true;         // settings.cfg + DFN_MENU=0 for tooling:
                                   // the tour and the playtest bot must not
                                   // stop at a menu nobody can press Enter on
    uint32_t start_stand = 0;      // DFN_MAP: which demo map when the menu is off
    float head_bob = 1.0f;         // settings.cfg: 0 disables bob/dip/settle
                                   // MOTION (events and sound still fire) --
                                   // the research's motion-sickness mandate
    bool palette_post = false; // Q9b palette quantization (DFN_PALETTE=1)
    std::string title_key = "app.title"; // localization key (Rule 5)

    // Populates the fields above from settings.cfg (auto-generated with
    // comments on first run; user-editable graphics settings per sync #3),
    // then applies DFN_* environment overrides on top (tour/tooling):
    // DFN_INTERNAL_RES=WxH, DFN_PALETTE=1, DFN_NULL_RENDER=1, DFN_NULL_PHYSICS=1.
    static AppConfig from_env();
};

class App {
public:
    App();
    ~App();
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    [[nodiscard]] bool init(const AppConfig& config);
    // Builds the world for one demo map. Called from init() when the menu is
    // off, or from the menu when the player picks a map.
    [[nodiscard]] bool enter_world(uint32_t stand);
    int run();
    void shutdown();

private:
    void pump_chunk_events(); // ferry ChunkLoaded/Unloaded -> render + physics

    // Menu-first launch: the engine is up but no world exists until a map is
    // chosen. Playing is the only mode that ticks the simulation.
    enum class AppMode : uint8_t { Menu, Playing };
    AppMode mode_ = AppMode::Playing;
    MenuModel menu_;
    uint32_t active_stand_ = 0;
    int menu_shot_frames_ = 0; // DFN_MENU_SHOT flush counter
    void body_probe_drive();  // fixed tick: pose the camera for the probe
    void body_probe_frame(float alpha, float frame_dt); // after render: shoot

    AppConfig config_{};

    std::unique_ptr<platform::IWindow> window_;
    std::unique_ptr<platform::IInput> input_;
    std::unique_ptr<platform::IRenderer> renderer_;
    std::unique_ptr<platform::IPhysics> physics_;
    std::unique_ptr<platform::IAudio> audio_;

    ecs::World world_;
    events::EventBus bus_;
    time::FixedTimestep timestep_;
    world::ChunkManager chunks_;
    render::RenderSystem render_system_;
    render::FirstPersonCamera camera_;
    render::Tour tour_;
    ecs::EntityId player_{};
    // In-game clock; DAY_LENGTH_SECONDS per day. Starts at START_TIME_OF_DAY
    // rather than at zero: zero is MIDNIGHT, so a fresh launch opened in the
    // dark and the frame gave no hint that the hour was the reason.
    double game_seconds_ = static_cast<double>(config::START_TIME_OF_DAY)
                           * static_cast<double>(config::DAY_LENGTH_SECONDS);
    std::array<platform::PhysicsBodyHandle, 4> world_edge_{}; // extent walls

    // Step feel + audio (sim's zone, wired here).
    platform::BusHandle sfx_bus_{};
    gameplay::StepSoundBank sound_bank_{};
    gameplay::WindLoop wind_loop_{};
    gameplay::StepContext step_ctx_{};

    // First-person body (character's zone, wired here).
    anim::Rig body_rig_{};
    ecs::EntityId mirror_puppet_{}; // DFN_MIRROR/DFN_SHOWCASE double, 0 when absent

    // BODY PROBE (Rule 27 evidence path for the body; DFN_BODY_PROBE=
    // stride|showcase|mirror). The screenshot Tour FREEZES the simulation, so
    // every animated subject in the project is invisible to it by construction:
    // no tick means no update_bodies, no stride clock, no clip reel. This probe
    // is the opposite instrument — the world RUNS and the camera is posed and
    // triggered off simulation state, so a frame can be demanded AT a named
    // stride phase or clip time. Debug tooling: gated, and it closes the app
    // when the shot list is spent.
    struct BodyProbe {
        std::string mode;            // stride | showcase | mirror
        std::string dir;             // output directory
        std::vector<float> targets;  // stride phase | clip time (s) | yaw offset
        size_t next = 0;             // index into targets
        int direction = 1;           // +1 targets ascend, -1 descend, 0 = cycle
        float warmup_s = 0.0f;       // streaming/settle time before the first shot
        float elapsed_s = 0.0f;
        float pitch = 0.0f;          // forced look pitch, radians
        float aim_yaw = 0.0f;        // resolved at warmup end (mirror/showcase)
        bool aimed = false;
        bool primed = false;         // one frame of history before triggering
        float value = 0.0f;          // this frame's tracked quantity
        float prev_value = 0.0f;     // last frame's, for crossing detection
        float tick_value = 0.0f;     // tracked quantity at the newest tick
        float prev_tick_value = 0.0f;
        int cooldown = 0;            // frames before another shot may be scheduled
        std::string log;             // one line per shot, written next to the frames
    };
    std::optional<BodyProbe> body_probe_;

    // Autonomous playtest (sim's zone; DFN_PLAYTEST=patrol|explore|soak).
    std::optional<gameplay::PlaytestState> playtest_;
    gameplay::PlaytestCheckEnv pt_env_{};
    std::string pt_dir_;
    int pt_shots_ = 0;
    bool pt_artifacts_pending_ = true;
};

} // namespace dfn::app
