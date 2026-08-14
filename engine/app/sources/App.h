/*
Created: 09:08:2026 - 00:45:00
Last updated: 14:08:2026 - 18:03:08
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
- 10:08:2026 - 19:26:40: Отладочный экран (F3) и снимок состояния (F2) с восстановлением по DFN_RESTORE — запрос пользователя: видеть куда смотрю, fps, скорость, координаты, и уметь передать состояние так, чтобы его подняли обратно.
- 10:08:2026 - 19:57:06: Поле счётчика попыток доводки восстановления.
- 10:08:2026 - 20:03:30: Счётчик попыток доводки более не используется — восстановление стало размещением.
- 10:08:2026 - 21:26:54: Поля признака тишины мира для затвора тура.
- 10:08:2026 - 22:37:21: hold_crouch_ -- a restored crouch survives the live keyboard, which is what makes an automated capture at full crouch possible at all (character's carve).
- 10:08:2026 - 23:32:21: Поле msaa_samples в настройках.
- 10:08:2026 - 23:51:30: Поля третьего лица и орбиты камеры.
- 11:08:2026 - 13:48:13: DFN_FRAME_LOG — по строке на каждый ПРЕДЪЯВЛЕННЫЙ кадр, без обратного чтения, без отстоя, без заморозки тика. Пользователь нашёл изъян нашего метода раньше нас: «при прогоне бега тряска есть, а в момент, когда делается скрин, тряски нет». Все наши двери съёмки гасят ровно то, на что наведены, поэтому дефект МЕЖДУ кадрами два дня приходил чистым. Первый же прогон дал размах fov_y 5.951° при беге против 0.0000° на ходьбе и стоя.
- 13:08:2026 - 17:21:38: Переправа мешей демо-предметов (геометрия sim, переправа здесь). Без неё три предмета появлялись с идентификатором меша, который никто не загрузил, и рисовались НИЧЕМ: дверь 1.8 × 2.0 м стояла невидимой в 2.5 м перед точкой старта, при том что луч попадал в её физическую коробку, наведение заполнялось честно и «Открыть» рисовалось поверх пустой травы.
- 13:08:2026 - 18:59:13: Состояние на момент, когда все восемь зон были остановлены случайным прерыванием. Дерево СОБИРАЕТСЯ; красными остаются пять тестов, каждый назван в сообщении коммита. Сохранено, чтобы работа зон не потерялась, а не потому, что она закончена.
- 13:08:2026 - 22:14:05: capture_after_frames_ — вторая единица счёта для той же двери снимка. Секунды несравнимы побитово: две руки одного рецепта на разной загрузке машины успевают разное число кадров.
- 14:08:2026 - 16:11:00: AppMode::Editor + свободная камера (EditorCamera). Новый режим летающей камеры (запрос пользователя В39/Л1): облёт мира не игроком; Tab вселяет камеру в игрока и обратно. Дверь DFN_EDITOR=1 (+DFN_EDITOR_CAM=x,y,z,yaw,pitch) — авто-прогон через дверь, не забирающий мышь.
- 14:08:2026 - 16:50:36: Браузер карт (контракт docs/MAP_LAYOUT.md): MapCatalog + current_manifest() (сим для зоны chat — путь чата из category/file_stem). Вход в Играть/Редактор открывает браузер; open_map() разрешает source (stand:/dfw:). Двери: DFN_OPEN_MAP=<кат>/<карта> грузит карту минуя браузер (взамен прежней DFN_EDITOR-в-мир; DFN_MAP занят render'ом), DFN_EDITOR=1 без карты открывает браузер редактора.
- 14:08:2026 - 17:36:02: Поля/методы чата, телеметрии и записи/повтора траектории (В28/O-серия): chat_pending_/chat_pending_entry_, write_pending_chat()+chat_path_for_current_map() (путь из current_manifest()), TelemetryRing telemetry_, TrajectoryRecorder/Player (O3). Включены ChatLog.h и TrajectoryRecord.h.
- 14:08:2026 - 17:51:15: Поле wireframe_ (клавиша 4/F4, каркас В28). Оверлеи редактора читают renderer_->frame_stats()/center_pick() напрямую.
- 14:08:2026 - 18:03:08: ChatOverlay chat_overlay_ (живое окно чата, В28) + include ChatOverlay.h. Открытие '/', ввод text_input(), Enter — отправка через write_pending_chat.
*/

#pragma once

#include "engine/anim/sources/Rig.h"
#include "engine/app/sources/ChatLog.h"
#include "engine/app/sources/ChatOverlay.h"
#include "engine/app/sources/DebugOverlay.h"
#include "engine/app/sources/EditorCamera.h"
#include "engine/app/sources/TrajectoryRecord.h"
#include "engine/app/sources/Menu.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/core/events/sources/EventBus.h"
#include "engine/core/time/sources/FixedTimestep.h"
#include "engine/gameplay/sources/PlayerMovement.h"
#include "engine/gameplay/sources/InteractableMesh.h"
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
#include <cstdio>
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
    uint32_t start_stand = 0;      // DFN_STAND: which demo map when the menu is off
    float head_bob = 1.0f;         // settings.cfg: 0 disables bob/dip/settle
                                   // MOTION (events and sound still fire) --
                                   // the research's motion-sickness mandate
    uint32_t msaa_samples = 4; // settings.cfg: coverage samples on the internal
                               // grid (0/1 off, 2, 4, 8). What stopped the
                               // treeline shimmer; DFN_MSAA overrides for tooling
    bool palette_post = false; // Q9b palette quantization (DFN_PALETTE=1)
    // settings.cfg min_brightness: the floor the final image never goes below
    // (0 = honest black). Turned live by the calibration screen. Default is one
    // palette shade step, because a default of zero would show the player, on
    // his first launch, exactly the darkness this setting exists to answer.
    float black_floor = static_cast<float>(config::BLACK_FLOOR_LEVEL);
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

    // The map currently loaded (whichever .map the browser opened, or the door
    // resolved). Carries category + file_stem + zone, from which a consumer
    // derives sibling paths -- the chat log lives at
    // assets/maps/<category>/<file_stem>.chat.jsonl (Rule 26 seam for the chat
    // zone). nullptr before any map is opened.
    [[nodiscard]] const MapManifest* current_manifest() const {
        return current_map_ ? &*current_map_ : nullptr;
    }

private:
    void pump_chunk_events(); // ferry ChunkLoaded/Unloaded -> render + physics

    // Menu-first launch: the engine is up but no world exists until a map is
    // chosen. Playing ticks the sim and drives the camera from the player's
    // CameraPose. Editor still ticks the sim (so streaming, sky and the body
    // keep living) but withholds the player's input and drives the camera from
    // a free EditorCamera instead -- a flying eye detached from the body.
    enum class AppMode : uint8_t { Menu, Playing, Editor };
    AppMode mode_ = AppMode::Playing;
    // Where Escape returns to when it opens the pause page: Playing or Editor.
    // Without it Resume always dropped back into Playing, so pausing the editor
    // and resuming would silently possess the body.
    AppMode paused_from_ = AppMode::Playing;
    // FREE CAMERA of the editor mode. Driven directly by the app each render
    // frame; never interpolated (the app owns the pose outright). Seeded from
    // the player eye on entry so the toggle in and out of the body is seamless.
    EditorCamera editor_cam_;
    // Enters the editor: seeds the free camera from the player's current eye
    // and switches mode. become_player_from_editor() does the reverse -- it
    // teleports the body's feet under the free camera and hands control back to
    // the Playing controller (the user's В39/Л1: "and the fly-over, and out of
    // the eyes, in the same field").
    void enter_editor_mode();
    void become_player_from_editor();
    // Resolves a browser-chosen .map to a world and builds it (source
    // stand:<id> -> the generator stand; source dfw:<file> -> the baked map,
    // which does not exist until the baker lands -- an honest on-screen status,
    // never a silent nothing, per docs/MAP_LAYOUT.md). Returns true when a
    // world was built; false leaves a browser_status for the player and stays
    // in the menu.
    [[nodiscard]] bool open_map(const MapManifest& manifest);
    MenuModel menu_;
    // The map browser's catalog, scanned from assets/maps at startup and
    // handed to the menu (which only reads it). App owns it; the menu borrows.
    MapCatalog catalog_;
    // The map that was actually opened (a copy of the chosen manifest), exposed
    // through current_manifest() for the chat zone's path derivation.
    std::optional<MapManifest> current_map_;
    uint32_t active_stand_ = 0;
    int menu_shot_frames_ = 0; // DFN_MENU_SHOT flush counter
    void body_probe_drive();  // fixed tick: pose the camera for the probe
    void body_probe_frame(float alpha, float frame_dt); // after render: shoot

    // DEBUG READOUT + STATE CAPTURE (user request). collect_snapshot() reads
    // the world; write_capture() saves the .png and its sidecar; apply_restore()
    // puts the player back where a sidecar says he was.
    [[nodiscard]] DebugSnapshot collect_snapshot(float alpha);
    void write_capture(const DebugSnapshot& snap);
    void apply_restore(const DebugSnapshot& snap);
    // CHAT BOX (В28/O-серия): writes the pending entry, with the current frame's
    // capture attached, into the map's chat. Serviced after render() for the
    // same reason F2 is -- the image and its record must be the same frame.
    void write_pending_chat(float alpha);
    // The chat file beside the ACTIVE map (docs/MAP_LAYOUT.md), derived from the
    // browser's current_manifest() (category/file_stem). "" when no map is open
    // (chat disabled, said once).
    [[nodiscard]] std::string chat_path_for_current_map() const;
    // THIRD PERSON (key 1), his request: a debug view from behind. Standing
    // still the mouse orbits the camera and the body does NOT turn; moving, the
    // camera locks behind him -- the Skyrim behaviour he named.
    bool third_person_ = false;
    float orbit_yaw_ = 0.0f;
    float orbit_pitch_ = 0.0f;
    bool debug_overlay_ = false;    // key 2 (F3 alias)
    // Whole-scene wireframe (В28), key 4 / F4. Toggles IRenderer::set_wireframe;
    // the editor overlay reads it back to label the mode. Off by default, zero
    // cost off (render's contract).
    bool wireframe_ = false;
    bool capture_pending_ = false;  // F2, serviced after render()
    FrameClock frame_clock_{};
    int captures_written_ = 0;
    std::string capture_dir_;
    double capture_after_s_ = 0.0;      // DFN_CAPTURE_AFTER, 0 = off
    double capture_after_elapsed_ = 0.0;
    // DFN_CAPTURE_AFTER_FRAMES, 0 = off. The SAME door counted in frames
    // instead of seconds, because the seconds door cannot be compared bit for
    // bit: two runs of one recipe reach different frame numbers under different
    // machine load, and everything derived from the frame counter then diverges.
    // Measured by ui: 4125 differing pixels between two runs on the same keys,
    // down to 412 once the sky's clocks were pinned -- and the remainder was
    // this. Frames are the unit the rest of the loop already runs on.
    uint64_t capture_after_frames_ = 0;
    uint64_t capture_after_frames_seen_ = 0;
    bool capture_then_close_ = false;
    int close_after_flush_ = 0; // frames to keep running so the PNG lands
    // FRAME LOG (DFN_FRAME_LOG=<path>) -- one line per PRESENTED frame, written
    // live, with no readback, no settle and no cooldown.
    //
    // Why it is not a screenshot: the user found the reason himself. "при
    // прогоне бега есть тряска, но в момент, когда делается скрин, тряски нет,
    // картинка статичная." Every capture door we own either freezes the tick
    // (the tour) or waits for the backend to flush (F2, the body probe's
    // cooldown of 4). A defect that lives in the DIFFERENCE between consecutive
    // frames cannot survive any of that -- the instrument settles the thing it
    // was pointed at. Two days of clean single frames were the instrument
    // agreeing with itself.
    //
    // So this logs the quantities that MOVE THE WHOLE PICTURE, once per frame
    // actually presented, and the between-frames motion is then arithmetic on
    // adjacent lines rather than something a still has to show.
    std::FILE* frame_log_ = nullptr;
    uint64_t frame_log_index_ = 0;
    // CHAT BOX (В28/O-серия; docs/MAP_LAYOUT.md). The chat is a JSONL append-log
    // beside the active map; the pending entry is written after render() so its
    // attached capture and the entry describe the same frame.
    bool chat_pending_ = false;
    ChatEntry chat_pending_entry_{};
    bool chat_then_close_ = false;       // the DFN_CHAT_MSG verification door closes
    // The typed-chat window (В28): opened with '/', it captures the keyboard for
    // live UTF-8 input; Enter sends (through write_pending_chat), Escape closes.
    ChatOverlay chat_overlay_;
    // TELEMETRY RING (item 3): sampled on the COUNTED clock in the editor and
    // flushed beside the map on stop. In-game stays light (В39: no continuous
    // log). Constructed in App() from config::TELEMETRY_RING_SAMPLES.
    TelemetryRing telemetry_;
    double telemetry_last_s_ = -1.0e18;  // counted-clock time of the last sample

    // TRAJECTORY RECORD + DETERMINISTIC REPLAY (O3, the key item of В28). Record
    // a walk/look per presented frame; replay drives the camera and the counted
    // clock from the file so two replays render bit-for-bit (Rule 53). Recording
    // is an editor action; replay is driven by R/P keys or the
    // DFN_TRAJ_REC / DFN_TRAJ_PLAY doors.
    TrajectoryRecorder traj_rec_;
    std::optional<TrajectoryPlayer> traj_play_;
    TrajectoryFrame replay_frame_{};      // the frame being replayed this iteration
    bool replaying_ = false;              // set per frame while a replay is live
    bool traj_play_then_close_ = false;   // the DFN_TRAJ_PLAY door closes when spent
    std::string traj_last_path_;          // last recording written (P replays it)
    std::string traj_rec_out_;            // DFN_TRAJ_REC target, "" = off
    bool traj_rec_arm_ = false;           // begin recording when the world is entered
    int traj_written_ = 0;                // names trajectory_NNN.dftraj
    // A restore read from DFN_RESTORE, held until enter_world() has built the
    // map it names -- the pose cannot be applied to a world that does not
    // exist yet, and the stand it names decides WHICH world gets built.
    std::optional<DebugSnapshot> restore_;
    // A RESTORED CROUCH IS HELD, not merely set once. accumulate_input rewrites
    // crouch_held from the real keyboard every RENDER frame, so a restored
    // crouch survived exactly until the first frame -- which is why no
    // automated capture had ever been taken at full crouch, and why the defect
    // that put the camera inside the chest was only ever seen by the user.
    // Cleared by any crouch capture that restores standing.
    bool hold_crouch_ = false;
    // Where a restore ASKED the capsule to end up. Checked once, the frame
    // after: IPhysics has no teleport, so a restore is a long collide-and-slide
    // walk and can be stopped by geometry. Reported, never assumed.
    std::optional<glm::vec3> restore_target_;
    // Remaining correction attempts. One collide-and-slide step does not carry
    // a long displacement (sim measured 0.53 m of residual), so the horizontal
    // correction is re-issued until it converges or these run out.
    int restore_attempts_ = 0; // vestigial after the teleport fix; kept at 0
    // STREAMING QUIESCENCE for the tour's settle (Rule 42). The tour waited a
    // fixed count of RENDERED frames for work denominated in SIM steps, and two
    // runs of the same binary differed by 17-35% of pixels because of it. These
    // let the app hold the countdown until the world has actually stopped
    // changing. `world_changed_this_frame_` is set by the chunk ferry and
    // cleared at the top of each frame.
    bool world_changed_this_frame_ = false;
    int quiet_frames_ = 0;         // consecutive settled frames (hysteresis)
    int tour_settle_frames_ = 0;   // frames since last settled; cap backstop

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
