/*
Created: 09:08:2026 - 00:45:00
Last updated: 18:08:2026 - 23:20:00
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
- 14:08:2026 - 19:14:02: Поля двери снимка (shot_after_frames_/_seen_) рядом с capture_after_*: та же единица счёта и тот же довод — кадры сравнимы побитово, стенные секунды нет. Отдельного флага закрытия не заведено, переиспользован chat_then_close_: снимок клавиши 5 И ЕСТЬ запись чата, значит и выключение то же.
- 14:08:2026 - 19:41:18: action_pressed() + include Controls.h — обработчики клавиш спрашивают привязку ПО ДЕЙСТВИЮ, а не называют Key здесь. Это и есть то, что не даёт экрану управления разъехаться с кодом. Половина от 83ef021: уехала в рабочем дереве, доезжает отдельно.
- 15:08:2026 - 01:04:30: gallery_objects_dir_ + gallery_bodies_ (полка реестра и твёрдые стволы).
- 15:08:2026 - 02:14:41: gallery_size_chunks_ — пролёт галереи из манифеста (колоссу нужен 2×2).
- 16:08:2026 - 21:08:52: gallery_scene_ — файл композиции следующей карты (пусто = старая
  автосетка).
- 16:08:2026 - 21:50:43: gallery_shelves_ — полки карты, уже разобранные из objects.
- 17:08:2026 - 07:05:56: scene_spawn_ / третье лицо по двери (см. App.cpp).
- 17:08:2026 - 10:00:40: SceneTile + scene_objects_ + refresh_scene_lod/bake_scene_tile.
- 17:08:2026 - 11:13:47: FireflyField живёт на КАРТУ, а не на чанк — рой у края стриминга не
  должен мигать (пользователь: «повсюду, а не только в какой-то зоне»).
- 17:08:2026 - 11:35:28: scene_doc_ — композиция текущей карты, прочитанная до земли.
- 17:08:2026 - 13:52:37: scene_collision_debug_ / collider_debug_ — DFN_DRAW_COLLIDERS.
- 17:08:2026 - 14:48:55: draw_bake_progress — экран первого запуска.
- 17:08:2026 - 16:27:55: AppConfig::fullscreen — режим, в котором рождается окно (settings.cfg).
- 17:08:2026 - 16:35:20: scene_dirty_ — правил ли кто-нибудь композицию в этой сессии.
- 17:08:2026 - 18:32:56: состояние руки строителя: палитра, призрак, приговор, цель удаления,
  запомненные мерки деталей. Решения — в BuildTool.{h,cpp}, здесь только провода.
- 17:08:2026 - 19:17:13: Поле editor_ui_ — каркас интерфейса редактора (EditorUi). Панелей этот файл не называет ни одной: они регистрируются сами через EditorUi::add_panel, и это то, что позволяет трём агентам добавлять инструменты в редактор, не правя втроём один файл.
- 17:08:2026 - 22:01:29: ЧЕТЫРЕ ОСТАЛЬНЫХ РЕЖИМА ПЕРЕСТАЛИ БЫТЬ ПУСТЫМИ (заказ 17.08 п.2:
  «состояние на R меняется, но инструменты не рисуются, не понятно что сейчас я
  делаю и что»). Поля: слой правок земли relief_, кисть рельефа и посадки,
  мазок, выбранная расстановка selected_ (это НЕ build_target_ — тот меняется
  от дрожания камеры, а править числами надо то, по чему ЩЁЛКНУЛИ), и имя
  текущей постройки build_group_name_. Решения — в EditorBrush/EditorPlant/
  BuildTool; здесь только провода, потому что этот файл держит окно и ничего
  собранного в нём измерить нельзя.
- 17:08:2026 - 22:32:14: flatten_written_ — один [pad] на МАЗОК, а не на кадр: pad это
  утверждение, которое композитор перечитывает, и шестьдесят штук в секунду
  похоронили бы файл, в котором ему жить.
- 18:08:2026 - 00:07:07: cam_trace_ — дверь DFN_CAM_TRACE=1: печатать в stderr
  пару «пришло смещение мыши / стал рыск» на каждом кадре редактора. Заведена
  не для отладки одного вечера: три захода подряд «камера не двигается»
  разбирал человек за игрой, потому что различить «мышь не дошла» и «камера
  проигнорировала» было нечем. Читается один раз при рождении App.
- 18:08:2026 - 01:54:26: ghost_uploaded_ — висит ли призрак В РЕНДЕРЕРЕ. Отдельно от build_ghost_:
  «что я держу» и «что загружено» — разные вопросы, и путать их значит оставлять
  деталь нарисованной после того, как её выпустили.
- 18:08:2026 - 12:07:50: ИНСТРУМЕНТЫ ПЕРЕЕХАЛИ В КЛАССЫ (docs/AUDIT_EDITOR_TOOLS.md).
  Отсюда ушли поля, которые были ЧУЖИМ состоянием, лежавшим у App: cursor_free_
  (теперь EditorToolbox::pointer_mode — клавиша R это часть контракта
  инструментов), terrain_brush_/plant_brush_/brush_stroke_/flatten_written_
  (у каждой кисти теперь своя, внутри своего инструмента) и build_open_
  (список объектов стал настройками инструмента постройки, а не отдельной
  панелью со своей клавишей). apply_terrain_dab принимает КИСТЬ аргументом:
  App больше не знает, какая кисть сейчас в руке, и не должен.
- 18:08:2026 - 13:08:07: ЗЕМЛЯ ДВИГАЕТСЯ, ПОКА ВЕДЁШЬ КИСТЬ, и ТРОПА КРИВОЙ — два заказа 18.08.
  Первый: перестройка чанка звалась только из finish_stroke, поэтому кисть вела по
  неподвижной земле («мне так непонятно что происходит»). Теперь показ идёт во время
  штриха с паузой, выведенной из ИЗМЕРЕННОЙ цены (196 мс на чанк) — StrokeRefresh.
  Второй: шестой инструмент (PathTool) и крючки под него — ground_height, relief_paths,
  commit_path, last_dab; линия и узлы рисуются из ToolPreview, без вопроса «что в руке».
  И ПОПУТНО ЗАКРЫТА СТАРАЯ ДЫРА: приложение НИ РАЗУ не звало read_relief/write_relief —
  ключ `relief` в .scene был, формат был, круговой прогон был, а правки земли жили до
  выхода из игры. Теперь сиделка читается со сценой и пишется кнопкой «сохранить».
- 18:08:2026 - 16:59:18: СЛОЙ 1 РАЗБОРА: dispatch_actions() и семнадцать методов on_*,
  определённых в AppInput.cpp. Поле force_third_person_ удалено — оно
  существовало только затем, чтобы дверь DFN_THIRD_PERSON попала в ту же ветку,
  что и клавиша; теперь дверь зовёт тот же метод, и промежуточный флаг стал
  лишним звеном (правило 32). unattended_run()/write_settings() объявлены здесь:
  обработчики клавиш уехали в соседний файл и зовут их оттуда.
- 18:08:2026 - 17:32:10: СЛОЙ 2: объявление unattended_run() отсюда убрано — оно
  живёт в AppDoors.h рядом с таблицей, из которой выводится.
- 18:08:2026 - 17:36:58: СЛОЙ 4: after_frame(alpha, dt) и два затвора вместо трёх
  голых счётчиков — FlushCountdown вместо close_after_flush_, SettleGate вместо
  quiet_frames_/tour_settle_frames_. Оба определены в AppAfterFrame.h заголовком
  и потому прогоняются рукавом без окна; здесь остаётся только их состояние.
- 18:08:2026 - 18:02:11: Поле house_ — постройка, которую правят три инструмента графа, и она же
  та модель, которой не хватало отмене. ОДНА на троих: копия у каждого
  инструмента — это три дома, расходящиеся на первом сдвинутом якоре.
- 18:08:2026 - 18:58:40: Объявлен on_axis_lock.
- 18:08:2026 - 20:26:30: Объявлен on_delete_selected.
- 18:08:2026 - 21:12:40: upload_house_mesh, seed_demo_house, версия залитого тела.
- 18:08:2026 - 21:38:05: Тело коллайдера постройки и буфер его вершин.
- 18:08:2026 - 23:20:00: nudge_selected_anchor, draw_editor_grid; сетка живёт в сессии, а не вторым полем здесь.
*/

#pragma once

#include "engine/anim/sources/Rig.h"
#include "engine/app/sources/ChatLog.h"
#include "engine/app/sources/ChatOverlay.h"
#include "engine/app/sources/BuildTool.h"
#include "engine/editor/sources/EditorPaletteView.h"
#include "engine/app/sources/AppAfterFrame.h"
#include "engine/app/sources/Controls.h"
#include "engine/editor/sources/EditorHistory.h"
#include "engine/app/sources/DebugOverlay.h"
#include "engine/app/sources/EditorCamera.h"
#include "engine/app/sources/EditorPlant.h"
#include "engine/editor/sources/EditorBrushView.h"
#include "engine/editor/sources/EditorPropsView.h"
#include "engine/editor/sources/EditorToolHouse.h"
#include "engine/editor/sources/EditorToolsBuiltin.h"
#include "engine/editor/sources/EditorUi.h"
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
#include "engine/render/sources/FloraFireflies.h"
#include "engine/render/sources/ObjectRegistry.h"
#include "engine/render/sources/RenderSystem.h"
#include "engine/render/sources/Tour.h"
#include "engine/world/sources/ChunkManager.h"
#include "engine/world/sources/Scene.h"

#include <array>
#include <map>
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
    // settings.cfg fullscreen: the mode the window is BORN in. F11 toggles it
    // live and writes the answer back, so the choice survives the session that
    // made it — a fullscreen key that forgets is a key you press every launch.
    bool fullscreen = false;
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

// unattended_run() ЖИВЁТ В AppDoors.h, вместе с таблицей дверей, из которой он
// и выводится. Здесь его объявления нет намеренно: второе объявление рядом с
// чужой таблицей — первый шаг ко второму ответу на тот же вопрос.
// Writes settings.cfg. Called from first run, from the calibration and
// settings pages, and from the fullscreen key -- which lives in AppInput.cpp
// since layer 1 of the App.cpp decomposition.
void write_settings(const AppConfig& cfg);

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
    /// The editor's ImGui frame. Panels (object menu, terrain brushes, the
    /// properties column) register themselves with it; this file names none of
    /// them on purpose — see the hook in run().
    EditorUi editor_ui_;
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

    // A KEY EDGE BY ACTION, resolved through the binding table (Controls.h)
    // instead of by naming a key here. That indirection is what keeps the
    // controls screen from drifting: a handler names what it DOES, the table
    // says which key does it, and the screen draws the same table.
    [[nodiscard]] bool action_pressed(Action action) const;

    // ЕДИНСТВЕННОЕ МЕСТО, ГДЕ КЛАВИША ВООБЩЕ ДОХОДИТ ДО ПРИЛОЖЕНИЯ (слой 1
    // разбора App.cpp, docs/PLAN_APP_DECOMPOSITION.md). Обходит таблицу
    // AppActions.h и зовёт метод, названный в её строке. Возвращает false,
    // когда кадр надо бросить (ESC увёл в меню паузы) — раньше на этом месте
    // стоял `continue` посреди тысячи строк.
    //
    // ЗАЧЕМ ВООБЩЕ ОДНО МЕСТО. До сегодня обработчиков было восемнадцать, и
    // каждый сам писал перед собой `!chat_typing &&`. Такой уговор соблюдают
    // все, пока не появится девятнадцатый; а цена ошибки — набранное в чате
    // слово, которое роняет снимки и вертит камеру. Теперь запрет — колонка
    // таблицы, и её читает рукав app_controls.
    [[nodiscard]] bool dispatch_actions(bool chat_typing);
    // ВЕСЬ ХВОСТ КАДРА, и у него ровно одна общая причина существовать там,
    // где он существует: всё это обязано идти ПОСЛЕ render(). Определён в
    // AppAfterFrame.cpp, довод — в шапке того файла.
    void after_frame(float alpha, float frame_dt);
    // Обработчики, названные строками таблицы. Определены в AppInput.cpp; имя
    // метода И ЕСТЬ поле `handler` в строке, и рукав держит их вместе.
    void on_third_person();
    void on_debug_readout();
    void on_state_capture();
    void on_wireframe();
    void on_screenshot();
    void on_toggle_body();
    void on_trajectory_record();
    void on_trajectory_replay();
    void on_chat_window();
    void on_quick_remark();
    void on_map();
    void on_menu_pause();
    void on_fullscreen();
    void on_cursor_toggle();
    void on_build_menu();
    void on_build_rotate();
    void on_undo_redo();
    void on_axis_lock();
    void on_delete_selected();
    /// Шаг стрелками по выбранному якорю; false — стрелки не нажаты.
    bool nudge_selected_anchor();
    /// Отсечки сетки вокруг прицела (только когда сетка включена).
    void draw_editor_grid(const ToolAim& aim);
    /// Пересчитать тело постройки и отдать его в отрисовку.
    void upload_house_mesh();
    /// Маленький сруб в графе — для беспилотного кадра (дверь DFN_HOUSE_DEMO).
    void seed_demo_house();
    void on_tool_pick(int index);
    // НЕ ДЕЙСТВИЕ, А ПОЛЛИНГ: стрелки крутят деталь, Delete её убирает. У них
    // нет строки в таблице привязок, потому что таблица — это КРАЙ клавиши, а
    // стрелки читаются как навигация меню в другом месте. Живут рядом с
    // обработчиками, потому что это тот же ввод и та же рука.
    void update_part_rotation();
    // Окно чата, пока в нём печатают: ввод, забой, отправка, закрытие. Отдельно
    // от таблицы, потому что это ветка, в которой таблица НЕ РАБОТАЕТ.
    void service_chat_typing();

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
    // DFN_SHOT_AFTER=<frames>: the dose door for the key-5 screenshot. Counted
    // in frames for the same reason its neighbour above is -- a wall second
    // holds a different number of frames on a loaded machine, so two runs of
    // one recipe would not be comparable. It reuses chat_then_close_ to exit:
    // the shot IS a chat entry, so it is the same shutdown.
    uint64_t shot_after_frames_ = 0;
    uint64_t shot_after_frames_seen_ = 0;
    // СКОЛЬКО КАДРОВ ЕЩЁ РИСОВАТЬ, ЧТОБЫ .PNG УСПЕЛ ЛЕЧЬ. Было голое число;
    // стало объект с правилом «второй взвод не укорачивает ожидание», потому
    // что снимок и запись чата приходятся на один кадр (клавиша 5 это и то и
    // другое). Правило проверяется в tests/app/AfterFrameTests.cpp.
    FlushCountdown flush_countdown_;
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
    // ЗАТВОР ТУРА: гистерезис и потолок, вынесенные в AppAfterFrame.h, где их
    // прогоняет рукав. Здесь было два счётчика и два литерала в кадровом цикле.
    SettleGate settle_gate_;

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
    /// Registry directory the NEXT Gallery open loads from (set by open_map
    /// from the manifest; default = the tree shelf) and the exhibits' static
    /// trunk bodies (user: «сделать деревья физичными, не давать сквозь них
    /// ходить»), destroyed on the next gallery load.
    std::string gallery_objects_dir_ = "assets/objects/trees";
    /// The map's .scene, if it has one: WHERE things stand, as an edited file
    /// instead of a grid this code invents. Empty = the auto-grid, as before.
    std::string gallery_scene_;
    /// ONE TILE OF A COMPOSITION: its placements, kept so the tile can be
    /// re-baked in a cheaper form when the player walks away from it, and the
    /// form it is currently baked in.
    struct SceneTile {
        glm::ivec2 key{0};
        glm::vec2 min_xz{0.0f};
        glm::vec2 max_xz{0.0f};
        std::vector<world::Placement> parts;
        bool far_form = false;
    };
    /// THE SWARM. Lives for the whole map, not for a chunk: the user asked for
    /// fireflies «повсюду, а не только в какой-то зоне», and a chunk-owned
    /// swarm would blink out at the streaming edge.
    /// The CURRENT map's composition, read once before the ground is built
    /// (its pads shape the height field) and used again to place the objects.
    /// Draws one frame of the first-run preparation screen.
    void draw_bake_progress(std::size_t done, std::size_t total,
                            const std::string& what);

    world::SceneDoc scene_doc_;
    /// HAS THE COMPOSITION BEEN CHANGED IN THIS SESSION? Saving writes the doc
    /// back over its .scene, and those files carry HAND-WRITTEN comments and
    /// hand-chosen ordering (the showcase says so in its own header). Writing
    /// an unchanged doc would silently reformat somebody's file and lose the
    /// comments — so "save" with nothing to save must be a refusal that says
    /// so, not a no-op and not a rewrite.
    bool scene_dirty_ = false;

    // ---- THE BUILD HAND (editor). Decisions live in BuildTool.{h,cpp}; what
    // is here is the state and the wiring to the world.
    std::vector<BuildGroup> build_groups_;
    /// МЕНЮ ОБЪЕКТОВ: модель живёт здесь, панель объявляется в EditorUi один
    /// раз при подъёме карты. App владеет обеими — панель на них ссылается.
    PaletteModel palette_;
    bool palette_wired_ = false;
    /// КУРСОР ЖИВЁТ В ЯЩИКЕ ИНСТРУМЕНТОВ (EditorToolbox::pointer_mode), а не
    /// здесь: клавиша R — часть контракта инструментов («почти как в vim»), и
    /// поле в App было бы второй копией того же состояния. Спрашивать —
    /// editor_ui_.toolbox().pointer_mode().
    /// DFN_CAM_TRACE=1 — печатать в stderr пару «пришло смещение мыши / стал
    /// рыск» на каждом кадре редактора. Читается один раз при старте.
    bool cam_trace_ = false;
    std::size_t build_group_ = 0;
    std::size_t build_item_ = 0;
    float build_yaw_ = 0.0f;
    BuildGhost build_ghost_;
    /// Висит ли сейчас призрак В РЕНДЕРЕРЕ. Отдельно от build_ghost_, потому
    /// что вопрос «что я держу» и вопрос «что загружено» — разные, и путать их
    /// значит оставлять деталь нарисованной после того, как её выпустили.
    bool ghost_uploaded_ = false;
    /// ИСТОРИЯ ПРАВОК: снимки состояния, а не обратные действия. Обратное
    /// действие требует, чтобы КАЖДАЯ операция умела себя обращать, и ломается
    /// на первой, которая не умеет, — а дальше отмена врёт молча.
    EditorHistory history_;
    /// ПОСТРОЙКА, КОТОРУЮ ПРАВЯТ ТРИ ИНСТРУМЕНТА (вершины, прямая, поверхность),
    /// и ОДНА на всех троих: копия графа у каждого — это три дома, которые
    /// разъедутся на первом же сдвинутом якоре. Здесь же она нужна отмене:
    /// история хранит снимки текстом и НЕ ЗНАЕТ про модель нарочно, поэтому
    /// применяет снимок тот, у кого модель есть, — и это единственное место.
    HouseSession house_;
    /// Версия геометрии, при которой тело залито последний раз.
    std::uint32_t house_mesh_version_ = 0;
    /// Сетка и её шаг живут в сессии постройки (HouseSession): по ней прилипают
    /// якоря, ею шагают стрелки, её же рисуют отсечки. Здесь поля НЕТ нарочно —
    /// второе такое поле было бы вторым ответом на один вопрос.
    /// Коллайдер постройки и его вершины. Позиции живут полем, а не временной
    /// переменной: дескриптор физики берёт их СПАНОМ, и буфер обязан пережить
    /// вызов, иначе тело построится по памяти, которой уже нет.
    platform::PhysicsBodyHandle house_body_{};
    std::vector<glm::vec3> house_positions_;
    BuildVerdict build_verdict_;
    /// Which placement the crosshair is on, for DELETING. npos = none. Kept as
    /// an index into scene_doc_.placements, resolved fresh every frame: an
    /// index remembered across an edit would delete the wrong thing.
    std::size_t build_target_ = static_cast<std::size_t>(-1);
    /// Measured sizes, memoised. Same ruler as the judge and the tools
    /// (render::measure_object), never a second copy.
    std::map<std::string, render::ObjectExtent> build_extents_;
    void update_build_tool();
    void clear_build_ghost();
    [[nodiscard]] bool build_place();
    [[nodiscard]] bool build_delete();

    // ---- THE OTHER FOUR TOOLS (editor). The mode lives in EditorUi; this is
    // what each mode DOES, and it is wiring only — every decision belongs to a
    // module a test can instantiate (EditorBrush, EditorPlant, BuildTool).
    //
    // WHY THEY ARE HERE AT ALL (user, 17.08.2026: «состояние на R меняется, но
    // инструменты не рисуются, не понятно что сейчас я делаю и что»). Four of
    // the five modes were empty: the chip lit up, the camera obeyed, and the
    // world did not answer. A mode that changes nothing outside the interface
    // is indistinguishable from a broken key.
    /// The hand edit of the ground for THIS map, and the one truth about it:
    /// it goes into the world through ChunkManager::set_composed_relief, which
    /// feeds compose_passes — the ground the player walks and the ground
    /// check_scene judges are then one thing.
    world::ReliefLayer relief_;
    /// Записать карту ЦЕЛИКОМ: сцену и сиделку .relief рядом с ней. Одна кнопка
    /// «сохранить» не обязана знать, что записей две.
    bool save_map_with_relief();
    /// ЕДИНСТВЕННАЯ ДВЕРЬ К ЗАПИСИ ТРОПЫ (ToolWorld::commit_path): добавить,
    /// заменить или убрать, перепечь канал износа и пометить землю.
    std::size_t commit_relief_path(std::size_t index, const world::ReliefPath* path);
    /// КИСТИ ЖИВУТ В СВОИХ ИНСТРУМЕНТАХ (HeightBrushTool / SurfacePaintTool /
    /// PlantTool). Общий TerrainBrush с полем mode, показанный двумя фишками,
    /// и был тем «странным взаимодействием покраски и высоты», которое
    /// пользователь назвал 18.08: полоса задавала режим, панель задавала его же
    /// из второго места. Два объекта не могут разойтись во мнении о своём
    /// режиме.
    /// The last dab's numbers, for the panel's readout. A brush that has
    /// silently stopped biting looks exactly like a brush aimed at nothing.
    int last_dab_samples_ = 0;
    float last_dab_worst_m_ = 0.0f;
    /// КОГДА ПОКАЗАТЬ ЗЕМЛЮ, НЕ ДОЖИДАЯСЬ ОТПУСКАНИЯ (заказ 18.08: «хочу
    /// изменение ландшафта от инструмента высоты в реальном времени... а мне
    /// так непонятно что происходит»). Решение живёт в engine/editor отдельным
    /// предметом, а не парой полей здесь, потому что App держит окно и не
    /// заводится в проверке — а вопрос «сколько раз за штрих изменилась земля»
    /// не задать ни одному кадру.
    StrokeRefresh stroke_refresh_;
    /// Земля действительно сдвинулась с прошлого показа. Не «кнопка зажата»:
    /// кисть, наведённая за край подгруженного кольца, не двигает ничего, и
    /// перестраивать после неё нечего.
    bool ground_moved_since_push_ = false;
    bool brush_wired_ = false;
    /// Species the map's shelves carry, read once — a directory listing per
    /// frame is a directory listing per frame.
    std::vector<std::string> plant_species_;
    /// THE SELECTED PLACEMENT, and it is NOT build_target_: that one is what
    /// the crosshair is over right now and changes as the camera drifts, which
    /// is the wrong thing to be editing numbers of. This one is what the
    /// builder CLICKED, and it survives him looking away from it.
    std::size_t selected_ = static_cast<std::size_t>(-1);
    bool props_wired_ = false;
    /// The properties column's live numbers, edited in place by the panel and
    /// pushed into the world through EditorPlant::edit_placement — which
    /// re-judges, and puts the placement back on a refusal.
    PropsModel props_;
    /// ЧТО СТРОИМ СЕЙЧАС — the group new parts join. Empty means "alone", and
    /// alone is what every hand-placed part used to be: see the note at
    /// build_place() for why that made a house impossible to build by hand.
    std::string build_group_name_;
    /// Declares the editor's panels ONCE, on entering the editor rather than on
    /// a keypress: a menu that does not exist until you press its shortcut is a
    /// menu for whoever wrote it.
    void wire_editor_panels();
    void update_editor_tools(float dt_s);
    /// Pushes the properties column's numbers into the composition, re-judged.
    /// False = the judge refused and nothing changed; props_.refusal says why.
    [[nodiscard]] bool apply_selection_edit();
    /// Where the crosshair meets the ground (or what it meets first). ONE aim
    /// for five tools — three copies of this march would drift the first time
    /// one of them was tuned, and the symptom is a brush biting a metre from
    /// the cross.
    [[nodiscard]] glm::vec3 editor_aim_point();
    /// ТО ЖЕ, НО С ДАЛЬНОСТЬЮ И ПРИЗНАКОМ ПОПАДАНИЯ — то, что читает потолок
    /// дальности (EditorToolbox). Голый vec3 не давал спросить «а как далеко»,
    /// поэтому общий параметр было негде проверить.
    [[nodiscard]] ToolAim editor_aim();
    /// КИСТЬ ПРИХОДИТ ОТ ИНСТРУМЕНТА, а не берётся из поля App: настройки,
    /// которые человек двигал, и земля, которую он копает, обязаны быть ОДНОЙ
    /// кистью.
    [[nodiscard]] bool apply_terrain_dab(const TerrainBrush& brush, glm::vec2 centre,
                                         float dt_s);
    void finish_stroke();
    [[nodiscard]] int plant_dab_here(const PlantBrush& brush, glm::vec2 centre);
    /// Re-bakes the ONE tile a placement falls in. An edit must not cost a
    /// whole-map re-bake: the builder places a part every few seconds.
    void rebake_tile_at(glm::vec2 world_xz);
    [[nodiscard]] const std::string& build_selected() const;
    /// DFN_DRAW_COLLIDERS=1: the collision triangles kept so the debug pass can
    /// draw them. Requested by the user after three separate "I cannot walk
    /// here" reports that all turned out to be one wrong collider — a shape
    /// nobody could see was a shape nobody could argue with.
    struct DebugCollision {
        std::vector<glm::vec3> positions;
        std::vector<uint32_t> indices;
    };
    std::vector<DebugCollision> scene_collision_debug_;
    bool collider_debug_ = false;
    render::FireflyField fireflies_;
    std::vector<SceneTile> scene_tiles_;
    /// Every registry object the composition uses, near forms and `-far` forms
    /// alike, keyed by the name that was read. Kept resident because a re-bake
    /// must not go back to disk: it happens while the player is walking.
    std::map<std::string, render::RegistryObject> scene_objects_;
    /// Re-bakes at most ONE composition tile per frame into the form its
    /// distance asks for. One per frame, nearest mismatch first: the same
    /// stance the scatter ladder takes, for the same reason — a re-bake costs
    /// a bake, and the mismatch that matters is the one in front of the eye.
    void refresh_scene_lod(glm::vec3 eye);
    /// Builds and uploads one tile in the given form.
    void bake_scene_tile(SceneTile& tile, bool far_form);

    /// Where the CURRENT map's composition wants the player, if it said so.
    /// Recorded while the scene loads and consumed by the single spawn call at
    /// the end of enter_world — never spawned on the spot, because everything
    /// after that call (the character rig above all) must still run.
    std::optional<glm::vec3> scene_spawn_;
    /// DFN_THIRD_PERSON, fired once through the ordinary toggle branch.
    bool third_person_door_fired_ = false;
    float scene_spawn_yaw_ = 0.0f;
    /// The shelf list from `objects`, already split on ';' and trimmed.
    std::vector<std::string> gallery_shelves_;
    int gallery_size_chunks_ = 1; // Gallery extent, from the manifest
    std::vector<platform::PhysicsBodyHandle> gallery_bodies_;

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
